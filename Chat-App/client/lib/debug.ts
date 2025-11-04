export const debugLog = (label: string, data: any) => {
  console.log(`[DEBUG] ${label}:`, JSON.stringify(data, null, 2));
};

export const makeJSONRequest = async (
  url: string,
  method: string,
  data?: any,
  headers?: any
) => {
  const fullUrl = url.startsWith('http') ? url : `http://localhost:3005${url}`;
  
  const requestInit: RequestInit = {
    method,
    headers: {
      'Content-Type': 'application/json',
      ...headers,
    },
  };

  if (data) {
    requestInit.body = JSON.stringify(data);
    debugLog(`${method} ${url} - Request Body`, data);
  }

  try {
    const response = await fetch(fullUrl, requestInit);
    const responseData = await response.json();
    
    debugLog(`${method} ${url} - Response [${response.status}]`, responseData);
    
    if (!response.ok) {
      throw new Error(responseData.message || `HTTP ${response.status}`);
    }
    
    return responseData;
  } catch (error: any) {
    console.error(`Error in ${method} ${url}:`, error);
    throw error;
  }
};
